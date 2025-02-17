#!/bin/bash
# git remote add iowarp https://github.com/iowarp/iowarp-runtime.git
# git remote add grc https://github.com/grc-iit/chimaera.git
gh pr create --title $1 --body "" --repo=grc-iit/chimaera
gh pr create --title $1 --body "" --repo=iowarp/iowarp-runtime
