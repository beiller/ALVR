#define IDR_FONT 1000
#define IDR_VS 1001
#define IDR_PS 1002
#define IDR_RECENTER_TEXTURE 1003
#define IDR_MESSAGE_BG_TEXTURE 1004

#define APP_VERSION_MAJOR 2
#define APP_VERSION_MINOR 2
#define APP_VERSION_PATCH 0
#define APP_VERSION_SUFFIX "-alpha"
#define APP_VERSION_STRING__(major, minor, patch, suffix) #major "." #minor "." #patch "" suffix
#define APP_VERSION_STRING_(major, minor, patch, suffix) APP_VERSION_STRING__(major, minor, patch, suffix)
#define APP_VERSION_STRING APP_VERSION_STRING_(APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH, APP_VERSION_SUFFIX)

#define APP_NAME "ALVR"
#define APP_MODULE_NAME "ALVR Server Driver"

#define APP_MUTEX_NAME "ALVR_DRIVER_MUTEX_67F10C68-E26D-4ABE-AE25-32BF346D877B"
#define APP_FILEMAPPING_NAME "ALVR_DRIVER_FILEMAPPING_0B124897-7730-4B84-AA32-088E9B92851F"

#define ALVR_FREEPIE_FILEMAPPING_NAME "ALVR_FREEPIE_FILEMAPPING_13B65572-591A-4248-A2F6-BAC2D89EE3B8"
#define ALVR_FREEPIE_MUTEX_NAME "ALVR_FREEPIE_MUTEX_AA77F1C3-86E4-4EF9-AAA2-5C40CF380D7A"
