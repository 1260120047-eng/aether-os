# Aether bash ayarları
[ -f /etc/profile.d/bash_completion.sh ] && . /etc/profile.d/bash_completion.sh
PS1='\[\e[38;5;141m\]\u\[\e[38;5;245m\]@\[\e[38;5;111m\]\h \[\e[38;5;252m\]\w \[\e[38;5;141m\]›\[\e[0m\] '
alias ls='ls --color=auto' ll='ls -lh --color=auto' la='ls -lAh --color=auto' grep='grep --color=auto'
alias guncelle='aether-guncelle'
