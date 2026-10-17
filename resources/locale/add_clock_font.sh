#!/bin/bash
# Adds the "CLOCK FONT" entry to every <lang>/emulationstation2.po next to this script.
# No arguments. Files that already contain the msgid are skipped.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MSGID="CLOCK FONT"

declare -A TR=(
	[br]="FONTE DO RELÓGIO"
	[de]="UHRSCHRIFTART"
	[el]="ΓΡΑΜΜΑΤΟΣΕΙΡΑ ΡΟΛΟΓΙΟΥ"
	[es]="FUENTE DEL RELOJ"
	[fr]="POLICE DE L'HORLOGE"
	[it]="CARATTERE OROLOGIO"
	[ja]="時計のフォント"
	[ko]="시계 글꼴"
	[pl]="CZCIONKA ZEGARA"
	[pt]="TIPO DE LETRA DO RELÓGIO"
	[ru]="ШРИФТ ЧАСОВ"
	[sv]="KLOCKTYPSNITT"
	[ua]="ШРИФТ ГОДИННИКА"
	[uk]="ШРИФТ ГОДИННИКА"
	[vi]="PHÔNG CHỮ ĐỒNG HỒ"
	[zh-CN]="时钟字体"
	[zh-TW]="時鐘字型"
)

printf '%-8s %-8s %s\n' "LOCALE" "RESULT" "DETAIL"
printf '%-8s %-8s %s\n' "------" "------" "------"

for lang in $(printf '%s\n' "${!TR[@]}" | sort); do
	po="$DIR/$lang/emulationstation2.po"

	if [ ! -f "$po" ]; then
		printf '%-8s %-8s %s\n' "$lang" "MISSING" "$po"
		continue
	fi

	if grep -qxF "msgid \"$MSGID\"" "$po"; then
		printf '%-8s %-8s %s\n' "$lang" "SKIP" "already present"
		continue
	fi

	# make sure the file ends with a newline before appending
	[ -n "$(tail -c1 "$po")" ] && echo >> "$po"

	printf '\n#:\nmsgid "%s"\nmsgstr "%s"\n' "$MSGID" "${TR[$lang]}" >> "$po"
	printf '%-8s %-8s %s\n' "$lang" "ADDED" "${TR[$lang]}"
done