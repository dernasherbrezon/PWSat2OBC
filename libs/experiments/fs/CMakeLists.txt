set(NAME exp_fs)

add_library(${NAME} STATIC        
    ExperimentFile.cpp
)

target_link_libraries(${NAME} PUBLIC
    fs
    time
    logger
    gsl
)

target_include_directories(${NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Include)
target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Include/fs) 