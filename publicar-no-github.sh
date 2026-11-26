#!/usr/bin/env bash
# Uso: ./publicar-no-github.sh   (precisa do GitHub CLI: https://cli.github.com  e de `gh auth login`)
set -e
git init -b main
git add .
git commit -m "THE VOC: versao inicial"
gh repo create THE-VOC --public --source=. --remote=origin --push
echo "Pronto! Veja a compilacao em: $(gh repo view --json url -q .url)/actions"
