# Add core source files to the engine target
target_sources(AngelBase PRIVATE
    AngelBase.cpp
    ExceptionHandler.cpp
    LogHandler.cpp
    ServiceLocator.cpp
)

# Make this folder available for includes
target_include_directories(AngelBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

