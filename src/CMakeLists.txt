### DO NOT DELETE THIS COMMENT: INSERT_ARCHETYPES_HERE ###
USE_CYCLUS("tricycle" "fusion_power_plant")
USE_CYCLUS("tricycle" "flexible_fusion_plant")

INSTALL_CYCLUS_MODULE("tricycle" "")

# install header files
FILE(GLOB h_files "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
