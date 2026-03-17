cmake_minimum_required(VERSION 3.30)

foreach (example logging validation middleware testing)
    add_executable(splice_example_${example} ${example}.cpp)

    target_link_libraries(splice_example_${example}
            PRIVATE splice::splice
    )

    target_compile_options(splice_example_${example}
            PRIVATE -freflection
    )
endforeach ()