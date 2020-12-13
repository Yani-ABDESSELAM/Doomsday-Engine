/** @file image.cpp  Image objects and related routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"
#include "resource/image.h"
#include "resource/tga.h"

#include <doomsday/resource/patch.h>
#include <doomsday/resource/colorpalettes.h>

#include <de/memory.h>
#include <de/LogBuffer>
#include <QByteArray>
#include <QImage>
#include <de/NativePath>
#include <doomsday/filesys/fs_main.h>

#include "dd_main.h"

#include <doomsday/res/Composite>
#include <doomsday/resource/pcx.h>

#include "gl/gl_tex.h"

#include "render/rend_main.h" // misc global vars awaiting new home

#ifndef DENG2_QT_4_7_OR_NEWER // older than 4.7?
#  define constBits bits
#endif

using namespace de;
using namespace res;

struct GraphicFileType
{
    /// Symbolic name of the resource type.
    String name;

    /// Known file extension.
    String ext;

    bool (*interpretFunc)(FileHandle &hndl, String filePath, image_t &img);

    //char const *(*getLastErrorFunc)(); ///< Can be NULL.
};

static bool interpretPcx(FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(img);
    img.pixels = PCX_Load(hndl, img.size, img.pixelSize);
    return (0 != img.pixels);
}

static bool interpretJpg(FileHandle &hndl, String /*filePath*/, image_t &img)
{
    return Image_LoadFromFileWithFormat(img, "JPG", hndl);
}

static bool interpretPng(FileHandle &hndl, String /*filePath*/, image_t &img)
{
    return Image_LoadFromFileWithFormat(img, "PNG", hndl);
}

static bool interpretTga(FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(img);
    img.pixels = TGA_Load(hndl, img.size, img.pixelSize);
    return img.pixels != nullptr;
}

// Graphic resource types.
static GraphicFileType const graphicTypes[] = {
    { "PNG",    "png",      interpretPng /*, 0*/ },
    { "JPG",    "jpg",      interpretJpg /*, 0*/ }, // TODO: add alternate "jpeg" extension
    { "TGA",    "tga",      interpretTga /*, TGA_LastError*/ },
    { "PCX",    "pcx",      interpretPcx /*, PCX_LastError*/ },
    { "",       "",         nullptr /*,            0*/ } // Terminate.
};

static GraphicFileType const *guessGraphicFileTypeFromFileName(String fileName)
{
    // The path must have an extension for this.
    String ext = fileName.fileNameExtension();
    if (!ext.isEmpty())
    {
        for (int i = 0; !graphicTypes[i].ext.isEmpty(); ++i)
        {
            GraphicFileType const &type = graphicTypes[i];
            if (!ext.compareWithoutCase(type.ext))
            {
                return &type;
            }
        }
    }
    return nullptr; // Unknown.
}

static void interpretGraphic(FileHandle &hndl, String filePath, image_t &img)
{
    // Firstly try the interpreter for the guessed resource types.
    GraphicFileType const *rtypeGuess = guessGraphicFileTypeFromFileName(filePath);
    if (rtypeGuess)
    {
        rtypeGuess->interpretFunc(hndl, filePath, img);
    }

    // If not yet interpreted - try each recognisable format in order.
    if (!img.pixels)
    {
        // Try each recognisable format instead.
        /// @todo Order here should be determined by the resource locator.
        for (int i = 0; !graphicTypes[i].name.isEmpty(); ++i)
        {
            GraphicFileType const *graphicType = &graphicTypes[i];

            // Already tried this?
            if (graphicType == rtypeGuess) continue;

            graphicTypes[i].interpretFunc(hndl, filePath, img);
            if (img.pixels) break;
        }
    }
}

/// @return  @c true if the file name in @a path ends with the "color key" suffix.
static inline bool isColorKeyed(String path)
{
    return path.fileNameWithoutExtension().endsWith("-ck", String::CaseInsensitive);
}

void Image_Init(image_t &img)
{
    img.size      = Vector2ui(0, 0);
    img.pixelSize = 0;
    img.flags     = 0;
    img.paletteId = 0;
    img.pixels    = 0;
}

void Image_InitFromImage(image_t &img, Image const &guiImage)
{
    img.size      = guiImage.size();
    img.pixelSize = guiImage.depth() / 8;
    img.flags     = 0;
    img.paletteId = 0;
    img.pixels    = reinterpret_cast<uint8_t *>(M_Malloc(guiImage.byteCount()));
    std::memcpy(img.pixels, guiImage.bits(), guiImage.byteCount());
}

void Image_ClearPixelData(image_t &img)
{
    M_Free(img.pixels); img.pixels = 0;
}

image_t::Size Image_Size(image_t const &img)
{
    return img.size;
}

String Image_Description(image_t const &img)
{
    return String("Dimensions:%1 Flags:%2 %3:%4")
               .arg(img.size.asText())
               .arg(img.flags)
               .arg(0 != img.paletteId? "ColorPalette" : "PixelSize")
               .arg(0 != img.paletteId? img.paletteId : img.pixelSize);
}

void Image_ConvertToLuminance(image_t &img, bool retainAlpha)
{
    LOG_AS("Image_ConvertToLuminance");

    uint8_t *alphaChannel = 0, *ptr = 0;

    // Is this suitable?
    if (0 != img.paletteId || (img.pixelSize < 3 && (img.flags & IMGF_IS_MASKED)))
    {
        LOG_RES_WARNING("Unknown paletted/masked image format");
        return;
    }

    long numPels = img.size.x * img.size.y;

    // Do we need to relocate the alpha data?
    if (retainAlpha && img.pixelSize == 4)
    {
        // Yes. Take a copy.
        alphaChannel = reinterpret_cast<uint8_t *>(M_Malloc(numPels));

        ptr = img.pixels;
        for (long p = 0; p < numPels; ++p, ptr += img.pixelSize)
        {
            alphaChannel[p] = ptr[3];
        }
    }

    // Average the RGB colors.
    ptr = img.pixels;
    for (long p = 0; p < numPels; ++p, ptr += img.pixelSize)
    {
        int min = de::min(ptr[0], de::min(ptr[1], ptr[2]));
        int max = de::max(ptr[0], de::max(ptr[1], ptr[2]));
        img.pixels[p] = (min == max? min : (min + max) / 2);
    }

    // Do we need to relocate the alpha data?
    if (alphaChannel)
    {
        std::memcpy(img.pixels + numPels, alphaChannel, numPels);
        img.pixelSize = 2;
        M_Free(alphaChannel);
        return;
    }

    img.pixelSize = 1;
}

void Image_ConvertToAlpha(image_t &img, bool makeWhite)
{
    Image_ConvertToLuminance(img);

    long total = img.size.x * img.size.y;
    for (long p = 0; p < total; ++p)
    {
        img.pixels[total + p] = img.pixels[p];
        if (makeWhite) img.pixels[p] = 255;
    }
    img.pixelSize = 2;
}

bool Image_HasAlpha(image_t const &img)
{
    LOG_AS("Image_HasAlpha");

    if (0 != img.paletteId || (img.flags & IMGF_IS_MASKED))
    {
        LOG_RES_WARNING("Unknown paletted/masked image format");
        return false;
    }

    if (img.pixelSize == 3)
    {
        return false;
    }

    if (img.pixelSize == 4)
    {
        long const numpels = img.size.x * img.size.y;
        uint8_t const *in = img.pixels;
        for (long i = 0; i < numpels; ++i, in += 4)
        {
            if (in[3] < 255)
            {
                return true;
            }
        }
    }

    return false;
}

uint8_t *Image_LoadFromFile(image_t &img, FileHandle &file)
{
    LOG_AS("Image_LoadFromFile");

    String filePath = file.file().composePath();

    Image_Init(img);
    interpretGraphic(file, filePath, img);

    // Still not interpreted?
    if (!img.pixels)
    {
        LOG_RES_XVERBOSE("\"%s\" unrecognized, trying fallback loader...",
                         NativePath(filePath).pretty());
        return 0; // Not a recognised format. It may still be loadable, however.
    }

    // How about some color-keying?
    if (isColorKeyed(filePath))
    {
        uint8_t *out = ApplyColorKeying(img.pixels, img.size.x, img.size.y, img.pixelSize);
        if (out != img.pixels)
        {
            // Had to allocate a larger buffer, free the old and attach the new.
            M_Free(img.pixels);
            img.pixels = out;
        }

        // Color keying is done; now we have 4 bytes per pixel.
        img.pixelSize = 4;
    }

    // Any alpha pixels?
    if (Image_HasAlpha(img))
    {
        img.flags |= IMGF_IS_MASKED;
    }

    LOG_RES_VERBOSE("Loaded image from file \"%s\", size %s")
            << NativePath(filePath).pretty() << img.size.asText();

    return img.pixels;
}

bool Image_LoadFromFileWithFormat(image_t &img, char const *format, FileHandle &hndl)
{
    LOG_AS("Image_LoadFromFileWithFormat");

    /// @todo There are too many copies made here. It would be best if image_t
    /// contained an instance of QImage. -jk

    // It is assumed that file's position stays the same (could be trying multiple interpreters).
    size_t initPos = hndl.tell();

    Image_Init(img);

    // Load the file contents to a memory buffer.
    QByteArray data;
    data.resize(hndl.length() - initPos);
    hndl.read(reinterpret_cast<uint8_t*>(data.data()), data.size());

    QImage image = QImage::fromData(data, format);
    if (image.isNull())
    {
        // Back to the original file position.
        hndl.seek(initPos, SeekSet);
        return false;
    }

    //LOG_TRACE("Loading \"%s\"...") << NativePath(hndl->file().composePath()).pretty();

    // Convert paletted images to RGB.
    if (image.colorCount())
    {
        image = image.convertToFormat(QImage::Format_ARGB32);
        DENG_ASSERT(!image.colorCount());
        DENG_ASSERT(image.depth() == 32);
    }

    // Swap the red and blue channels for GL.
    image = image.rgbSwapped();

    img.size      = Vector2ui(image.width(), image.height());
    img.pixelSize = image.depth() / 8;

    LOGDEV_RES_VERBOSE("size:%s depth:%i alpha:%b bytes:%i")
            << img.size.asText() << img.pixelSize
            << image.hasAlphaChannel() << image.byteCount();

    img.pixels = reinterpret_cast<uint8_t *>(M_MemDup(image.constBits(), image.byteCount()));

    // Back to the original file position.
    hndl.seek(initPos, SeekSet);
    return true;
}

bool Image_Save(image_t const &img, char const *filePath)
{
    // Compose the full path.
    String fullPath = String(filePath);
    if (fullPath.isEmpty())
    {
        static int n = 0;
        fullPath = String("image%1x%2-%3").arg(img.size.x).arg(img.size.y).arg(n++, 3);
    }

    if (fullPath.fileNameExtension().isEmpty())
    {
        fullPath += ".png";
    }

    // Swap red and blue channels then save.
    QImage image = QImage(img.pixels, img.size.x, img.size.y, QImage::Format_ARGB32);
    image = image.rgbSwapped();

    return image.save(NativePath(fullPath));
}

uint8_t *GL_LoadImage(image_t &image, String nativePath)
{
    try
    {
        // Relative paths are relative to the native working directory.
        String path = (NativePath::workPath() / NativePath(nativePath).expand()).withSeparators('/');

        FileHandle &hndl = App_FileSystem().openFile(path, "rb");
        uint8_t *pixels = Image_LoadFromFile(image, hndl);

        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;

        return pixels;
    }
    catch (FS1::NotFoundError const&)
    {} // Ignore error.

    return 0; // Not loaded.
}

Source GL_LoadExtImage(image_t &image, char const *_searchPath, gfxmode_t mode)
{
    DENG_ASSERT(_searchPath);

    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri(RC_GRAPHIC, _searchPath),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        if (GL_LoadImage(image, foundPath))
        {
            // Force it to grayscale?
            if (mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
            {
                Image_ConvertToAlpha(image, mode == LGM_WHITE_ALPHA);
            }
            else if (mode == LGM_GRAYSCALE)
            {
                Image_ConvertToLuminance(image);
            }

            return External;
        }
    }
    catch (FS1::NotFoundError const&)
    {} // Ignore this error.

    return None;
}

static dd_bool palettedIsMasked(uint8_t const *pixels, int width, int height)
{
    DENG2_ASSERT(pixels != 0);
    // Jump to the start of the alpha data.
    pixels += width * height;
    for (int i = 0; i < width * height; ++i)
    {
        if (255 != pixels[i])
        {
            return true;
        }
    }
    return false;
}

static Source loadExternalTexture(image_t &image, String encodedSearchPath,
    String optionalSuffix = "")
{
    // First look for a version with an optional suffix.
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri(encodedSearchPath + optionalSuffix, RC_GRAPHIC),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        return GL_LoadImage(image, foundPath)? External : None;
    }
    catch (FS1::NotFoundError const&)
    {} // Ignore this error.

    // Try again without the suffix?
    if (!optionalSuffix.empty())
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri(encodedSearchPath, RC_GRAPHIC),
                                                         RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
            // Ensure the found path is absolute.
            foundPath = App_BasePath() / foundPath;

            return GL_LoadImage(image, foundPath)? External : None;
        }
        catch (FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    return None;
}

/**
 * Draw the component image @a src into the composite @a dst.
 *
 * @param dst               The composite buffer (drawn to).
 * @param dstDimensions     Pixel dimensions of @a dst.
 * @param src               The component image to be composited (read from).
 * @param srcDimensions     Pixel dimensions of @a src.
 * @param origin            Coordinates (topleft) in @a dst to draw @a src.
 *
 * @todo Optimize: Should be redesigned to composite whole rows -ds
 */
static void compositePaletted(dbyte *dst, Vector2ui const &dstDimensions,
    IByteArray const &src, Vector2ui const &srcDimensions, Vector2i const &origin)
{
    if (dstDimensions == Vector2ui()) return;
    if (srcDimensions == Vector2ui()) return;

    int const       srcW = srcDimensions.x;
    int const       srcH = srcDimensions.y;
    size_t const srcPels = srcW * srcH;

    int const       dstW = dstDimensions.x;
    int const       dstH = dstDimensions.y;
    size_t const dstPels = dstW * dstH;

    int dstX, dstY;

    for (int srcY = 0; srcY < srcH; ++srcY)
    for (int srcX = 0; srcX < srcW; ++srcX)
    {
        dstX = origin.x + srcX;
        dstY = origin.y + srcY;
        if (dstX < 0 || dstX >= dstW) continue;
        if (dstY < 0 || dstY >= dstH) continue;

        dbyte srcAlpha;
        src.get(srcY * srcW + srcX + srcPels, &srcAlpha, 1);
        if (srcAlpha)
        {
            src.get(srcY * srcW + srcX, &dst[dstY * dstW + dstX], 1);
            dst[dstY * dstW + dstX + dstPels] = srcAlpha;
        }
    }
}

/// Returns a palette translation id for the given class and map.
/// Note that a zero-length id is returned when @a tclass =0 and @a tmap =0
static String toTranslationId(int tclass, int tmap)
{
#define NUM_TRANSLATION_CLASSES         3
#define NUM_TRANSLATION_MAPS_PER_CLASS  7

    // Is translation unnecessary?
    if (!tclass && !tmap) return String();

    int trans = de::max(0, NUM_TRANSLATION_MAPS_PER_CLASS * tclass + tmap - 1);
    LOGDEV_RES_XVERBOSE("tclass=%i tmap=%i => TransPal# %i", tclass << tmap << trans);
    return String::number(trans);

#undef NUM_TRANSLATION_MAPS_PER_CLASS
#undef NUM_TRANSLATION_CLASSES
}

static Block loadAndTranslatePatch(IByteArray const &data, colorpaletteid_t palId,
    int tclass = 0, int tmap = 0)
{
    res::ColorPalette &palette = App_Resources().colorPalettes().colorPalette(palId);
    if (res::ColorPaletteTranslation const *xlat = palette.translation(toTranslationId(tclass, tmap)))
    {
        return res::Patch::load(data, *xlat, res::Patch::ClipToLogicalDimensions);
    }
    else
    {
        return res::Patch::load(data, res::Patch::ClipToLogicalDimensions);
    }
}

static Source loadPatch(image_t &image, FileHandle &hndl, int tclass = 0,
    int tmap = 0, int border = 0)
{
    LOG_AS("image_t::loadPatch");

    if (Image_LoadFromFile(image, hndl))
    {
        return External;
    }

    File1 &file = hndl.file();
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());

    // A DOOM patch?
    if (Patch::recognize(fileData))
    {
        try
        {
            colorpaletteid_t colorPaletteId = App_Resources().colorPalettes().defaultColorPalette();

            Block patchImg = loadAndTranslatePatch(fileData, colorPaletteId, tclass, tmap);
            PatchMetadata info = Patch::loadMetadata(fileData);

            Image_Init(image);
            image.size      = Vector2ui(info.logicalDimensions.x + border*2,
                                        info.logicalDimensions.y + border*2);
            image.pixelSize = 1;
            image.paletteId = colorPaletteId;

            image.pixels = (uint8_t *) M_Calloc(2 * image.size.x * image.size.y);

            compositePaletted(image.pixels, image.size,
                              patchImg, info.logicalDimensions, Vector2i(border, border));

            if (palettedIsMasked(image.pixels, image.size.x, image.size.y))
            {
                image.flags |= IMGF_IS_MASKED;
            }

            return Original;
        }
        catch (IByteArray::OffsetError const &)
        {
            LOG_RES_WARNING("File \"%s:%s\" does not appear to be a valid Patch")
                << NativePath(file.container().composePath()).pretty()
                << NativePath(file.composePath()).pretty();
        }
    }

    file.unlock();
    return None;
}

static Source loadPatchComposite(image_t &image, Texture const &tex,
    bool maskZero = false, bool useZeroOriginIfOneComponent = false)
{
    LOG_AS("image_t::loadPatchComposite");

    Image_Init(image);
    image.pixelSize = 1;
    image.size      = Vector2ui(tex.width(), tex.height());
    image.paletteId = App_Resources().colorPalettes().defaultColorPalette();

    image.pixels = (uint8_t *) M_Calloc(2 * image.size.x * image.size.y);

    res::Composite const &texDef = *reinterpret_cast<res::Composite *>(tex.userDataPointer());
    DENG2_FOR_EACH_CONST(res::Composite::Components, i, texDef.components())
    {
        File1 &file           = App_FileSystem().lump(i->lumpNum());
        ByteRefArray fileData = ByteRefArray(file.cache(), file.size());

        // A DOOM patch?
        if (Patch::recognize(fileData))
        {
            try
            {
                Patch::Flags loadFlags;
                if (maskZero) loadFlags |= Patch::MaskZero;

                Block patchImg     = Patch::load(fileData, loadFlags);
                PatchMetadata info = Patch::loadMetadata(fileData);

                Vector2i origin = i->origin();
                if (useZeroOriginIfOneComponent && texDef.componentCount() == 1)
                {
                    origin = Vector2i(0, 0);
                }

                // Draw the patch in the buffer.
                compositePaletted(image.pixels, image.size,
                                  patchImg, info.dimensions, origin);
            }
            catch (IByteArray::OffsetError const &)
            {} // Ignore this error.
        }

        file.unlock();
    }

    if (maskZero || palettedIsMasked(image.pixels, image.size.x, image.size.y))
    {
        image.flags |= IMGF_IS_MASKED;
    }

    // For debug:
    // GL_DumpImage(&image, Str_Text(GL_ComposeCacheNameForTexture(tex)));

    return Original;
}

static Source loadFlat(image_t &image, FileHandle &hndl)
{
    if (Image_LoadFromFile(image, hndl))
    {
        return External;
    }

    // A DOOM flat.
    Image_Init(image);

    /// @todo not all flats are 64x64!
    image.size      = Vector2ui(64, 64);
    image.pixelSize = 1;
    image.paletteId = App_Resources().colorPalettes().defaultColorPalette();

    File1 &file   = hndl.file();
    size_t fileLength = hndl.length();

    size_t bufSize = de::max(fileLength, (size_t) image.size.x * image.size.y);
    image.pixels = (uint8_t *) M_Malloc(bufSize);

    if (fileLength < bufSize)
    {
        std::memset(image.pixels, 0, bufSize);
    }

    // Load the raw image data.
    file.read(image.pixels, 0, fileLength);
    return Original;
}

static Source loadDetail(image_t &image, FileHandle &hndl)
{
    if (Image_LoadFromFile(image, hndl))
    {
        return Original;
    }

    // It must be an old-fashioned "raw" image.
    Image_Init(image);

    // How big is it?
    File1 &file = hndl.file();
    size_t fileLength = hndl.length();
    switch (fileLength)
    {
    case 256 * 256: image.size.x = image.size.y = 256; break;
    case 128 * 128: image.size.x = image.size.y = 128; break;
    case  64 *  64: image.size.x = image.size.y =  64; break;
    default:
        throw Error("image_t::loadDetail", "Must be 256x256, 128x128 or 64x64.");
    }

    image.pixelSize = 1;
    size_t bufSize = (size_t) image.size.x * image.size.y;
    image.pixels = (uint8_t *) M_Malloc(bufSize);

    if (fileLength < bufSize)
    {
        std::memset(image.pixels, 0, bufSize);
    }

    // Load the raw image data.
    file.read(image.pixels, fileLength);
    return Original;
}

Source GL_LoadSourceImage(image_t &image, ClientTexture const &tex,
                          TextureVariantSpec const &spec)
{
    de::FS1 &fileSys = App_FileSystem();
    auto &cfg = R_Config();

    Source source = None;
    variantspecification_t const &vspec = spec.variant;
    if (!tex.manifest().schemeName().compareWithoutCase("Textures"))
    {
        // Attempt to load an external replacement for this composite texture?
        if (cfg.noHighResTex->value().isFalse() &&
                (loadExtAlways || cfg.highResWithPWAD->value().isTrue() || !tex.isFlagged(Texture::Custom)))
        {
            // First try the textures scheme.
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if (source == None)
        {
            if (TC_SKYSPHERE_DIFFUSE != vspec.context)
            {
                source = loadPatchComposite(image, tex);
            }
            else
            {
                bool const zeroMask = (vspec.flags & TSF_ZEROMASK) != 0;
                bool const useZeroOriginIfOneComponent = true;
                source = loadPatchComposite(image, tex, zeroMask, useZeroOriginIfOneComponent);
            }
        }
    }
    else if (!tex.manifest().schemeName().compareWithoutCase("Flats"))
    {
        // Attempt to load an external replacement for this flat?
        if (cfg.noHighResTex->value().isFalse() &&
                (loadExtAlways || cfg.highResWithPWAD->value().isTrue() || !tex.isFlagged(Texture::Custom)))
        {
            // First try the flats scheme.
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");

            if (source == None)
            {
                // How about the old-fashioned "flat-name" in the textures scheme?
                source = loadExternalTexture(image, "Textures:flat-" + uri.path().toStringRef(), "-ck");
            }
        }

        if (source == None)
        {
            if (tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if (!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t const lumpNum = resourceUri.path().toString().toInt();
                        FileHandle &hndl    = fileSys.openLump(fileSys.lump(lumpNum));

                        source = loadFlat(image, hndl);

                        fileSys.releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch (LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if (!tex.manifest().schemeName().compareWithoutCase("Patches"))
    {
        int tclass = 0, tmap = 0;
        if (vspec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            tclass = vspec.tClass;
            tmap   = vspec.tMap;
        }

        // Attempt to load an external replacement for this patch?
        if (cfg.noHighResTex->value().isFalse() &&
                (loadExtAlways || cfg.highResWithPWAD->value().isTrue() || !tex.isFlagged(Texture::Custom)))
        {
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if (source == None)
        {
            if (tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if (!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t const lumpNum = resourceUri.path().toString().toInt();
                        FileHandle &hndl    = fileSys.openLump(fileSys.lump(lumpNum));

                        source = loadPatch(image, hndl, tclass, tmap, vspec.border);

                        fileSys.releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch (LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if (!tex.manifest().schemeName().compareWithoutCase("Sprites"))
    {
        int tclass = 0, tmap = 0;
        if (vspec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            tclass = vspec.tClass;
            tmap   = vspec.tMap;
        }

        // Attempt to load an external replacement for this sprite?
        if (cfg.noHighResPatches->value().isFalse())
        {
            de::Uri uri = tex.manifest().composeUri();

            // Prefer psprite or translated versions if available.
            if (TC_PSPRITE_DIFFUSE == vspec.context)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path() + "-hud", "-ck");
            }
            else if (tclass || tmap)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path() + String("-table%1%2").arg(tclass).arg(tmap), "-ck");
            }

            if (!source)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path(), "-ck");
            }
        }

        if (source == None)
        {
            if (tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if (!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t const lumpNum = resourceUri.path().toString().toInt();
                        FileHandle &hndl    = fileSys.openLump(fileSys.lump(lumpNum));

                        source = loadPatch(image, hndl, tclass, tmap, vspec.border);

                        fileSys.releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch (LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if (!tex.manifest().schemeName().compareWithoutCase("Details"))
    {
        if (tex.manifest().hasResourceUri())
        {
            de::Uri resourceUri = tex.manifest().resourceUri();
            if (resourceUri.scheme().compareWithoutCase("Lumps"))
            {
                source = loadExternalTexture(image, resourceUri.compose());
            }
            else
            {
                lumpnum_t const lumpNum = fileSys.lumpNumForName(resourceUri.path());
                try
                {
                    File1 &lump = fileSys.lump(lumpNum);
                    FileHandle &hndl = fileSys.openLump(lump);

                    source = loadDetail(image, hndl);

                    fileSys.releaseFile(hndl.file());
                    delete &hndl;
                }
                catch (LumpIndex::NotFoundError const&)
                {} // Ignore this error.
            }
        }
    }
    else
    {
        if (tex.manifest().hasResourceUri())
        {
            de::Uri resourceUri = tex.manifest().resourceUri();
            source = loadExternalTexture(image, resourceUri.compose());
        }
    }
    return source;
}
