if [ -z "$(ls -A ./messaging_system)" ]
then
    git submodule update --init
fi
git submodule update --remote

./messaging_system/dependency.sh