// macros
#define GRID_W (pixelW * pixelGrid)
#define GRID_H (pixelH * pixelGrid)
#define ACTIVECOLOR {"(profilenamespace getvariable ['GUI_BCG_RGB_R',0.77])","(profilenamespace getvariable ['GUI_BCG_RGB_G',0.51])","(profilenamespace getvariable ['GUI_BCG_RGB_B',0.08])", 1}
#define ACTIVECOLOR_SCRIPT [(profilenamespace getvariable ['GUI_BCG_RGB_R',0.77]),(profilenamespace getvariable ['GUI_BCG_RGB_G',0.51]),(profilenamespace getvariable ['GUI_BCG_RGB_B',0.08]), 1]
#define QUOTE(var1) #var1
#define SPACING 1
#define MAP_ITEM_W 30
#define MAP_ITEM_H 40

#define LOADING_STEP_H 3

// idc
#define IDC_MAPLIST 427
#define IDC_MAPITEM_PICTURE 2
#define IDC_MAPITEM_NAME 3
#define IDC_MAPITEM_SELECTINDICATOR 4
#define IDC_MAPITEM_AUTHOR 5

#define IDC_LOADINGLIST 742
#define IDC_LOADINGITEM_NAME 2
#define IDC_LOADINGITEM_STEP_READWRP 3
#define IDC_LOADINGITEM_STEP_SATIMAGE 4
#define IDC_LOADINGITEM_STEP_HOUSES 5
#define IDC_LOADINGITEM_STEP_PREVIEWIMGE 6
#define IDC_LOADINGITEM_STEP_META 7
#define IDC_LOADINGITEM_STEP_DEM 8


#define IDC_LOADINGSTEP_PICTURE 1337
#define IDC_LOADINGSTEP_TEXT 133742