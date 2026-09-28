Import("env")

flags = list(env.get("UPLOADERFLAGS", []))

try:
    write_flash_index = flags.index("write-flash")
except ValueError:
    write_flash_index = -1

if write_flash_index >= 0 and "--no-progress" not in flags:
    flags.insert(
        write_flash_index + 1,
        "--no-progress"
    )
    env.Replace(
        UPLOADERFLAGS=flags
    )
