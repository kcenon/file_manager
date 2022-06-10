#!/bin/bash
if [ -z "$(ls -A ./messaging_system)" ]; then
    git submodule update --init
fi

if [ ! -z "$1" ]; then
    if [ "$1" == "--submodule" ]; then
        git submodule foreach "git fetch && git reset --hard origin/main"
    fi
fi

/bin/bash ./messaging_system/dependency.sh
