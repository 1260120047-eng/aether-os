# Aether kabuk ayarları
if [ -z "$LANG" ]; then
  if grep -q '^DIL=en' "$HOME/.config/aether/ayarlar" 2>/dev/null; then export LANG=en_US.UTF-8; else export LANG=tr_TR.UTF-8; fi
fi
export EDITOR=vi
