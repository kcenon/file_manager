@ECHO OFF

if not exist ./messaging_system/ (
    git submodule update --init
)
git submodule foreach "git fetch && git reset --hard origin/main"

call ./messaging_system/dependency.bat
