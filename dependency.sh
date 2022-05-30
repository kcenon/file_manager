if [ -z "$(ls -A ./messaging_system)" ]
then
    git submodule update --init
fi
git submodule foreach "git fetch && git reset --hard origin/main"

./messaging_system/dependency.sh