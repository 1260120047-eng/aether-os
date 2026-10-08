# Aether bash ayarları
[ -f /etc/profile.d/bash_completion.sh ] && . /etc/profile.d/bash_completion.sh
PS1='\[\e[38;5;141m\]\u\[\e[38;5;245m\]@\[\e[38;5;111m\]\h \[\e[38;5;252m\]\w \[\e[38;5;141m\]›\[\e[0m\] '
alias ls='ls --color=auto' ll='ls -lh --color=auto' la='ls -lAh --color=auto' grep='grep --color=auto'
alias guncelle='aether-guncelle'
# Aether Terminal: sekme başlığında klasör adı, yeni sekme aynı klasörde açılsın
if [ -n "$VTE_VERSION" ]; then
  [ -f /etc/profile.d/vte.sh ] && . /etc/profile.d/vte.sh
  __aether_baslik() { printf '\033]0;%s\007' "${PWD/#$HOME/\~}"; }
  PROMPT_COMMAND="__aether_baslik${PROMPT_COMMAND:+; $PROMPT_COMMAND}"
fi
