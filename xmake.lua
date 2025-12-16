add_rules("mode.release", "mode.debug")
add_requires("minhook")

target("LuxorLauncher")
  set_languages("cxx20")
  set_exceptions("cxx")

  set_symbols("debug")

  add_files("./main.cpp")

  add_ldflags("/subsystem:windows", {force = true})

target_end()