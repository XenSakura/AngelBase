# Add core source files to the engine target
target_sources(KitsuneEngine PRIVATE
    KitsuneEngine.cpp
)

# Make this folder available for includes
target_include_directories(KitsuneEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

