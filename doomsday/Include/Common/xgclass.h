#ifndef __XG_LINE_CLASSES_H__
#define __XG_LINE_CLASSES_H__

enum							   // Line type classes.
{
	LTC_NONE,					   // No action.
	LTC_CHAIN_SEQUENCE,
	LTC_PLANE_MOVE,
	LTC_BUILD_STAIRS,
	LTC_DAMAGE,
	LTC_POWER,
	LTC_LINE_TYPE,
	LTC_SECTOR_TYPE,
	LTC_SECTOR_LIGHT,
	LTC_ACTIVATE,
	LTC_KEY,
	LTC_MUSIC,					   // Change the music to play.
	LTC_LINE_COUNT,				   // Line activation count delta.
	LTC_END_LEVEL,
	LTC_DISABLE_IF_ACTIVE,
	LTC_ENABLE_IF_ACTIVE,
	LTC_EXPLODE,				   // Explodes the activator.
	LTC_PLANE_TEXTURE,
	LTC_WALL_TEXTURE,
	LTC_COMMAND,
	LTC_SOUND,					   // Play a sector sound.
	LTC_MIMIC_SECTOR,
	NUM_LINE_CLASSES
};

enum {
	TXT_XGCLASS000,
	TXT_XGCLASS001,
	TXT_XGCLASS002,
	TXT_XGCLASS003,
	TXT_XGCLASS004,
	TXT_XGCLASS005,
	TXT_XGCLASS006,
	TXT_XGCLASS007,
	TXT_XGCLASS008,
	TXT_XGCLASS009,
	TXT_XGCLASS010,
	TXT_XGCLASS011,
	TXT_XGCLASS012,
	TXT_XGCLASS013,
	TXT_XGCLASS014,
	TXT_XGCLASS015,
	TXT_XGCLASS016,
	TXT_XGCLASS017,
	TXT_XGCLASS018,
	TXT_XGCLASS019,
	TXT_XGCLASS020,
	TXT_XGCLASS021,
	NUMXGCLASSES
};

#define CLASSNAMELENGTH 21		// 20 characters max in a class name + terminator

extern char xgClassNames[][CLASSNAMELENGTH];

#endif
