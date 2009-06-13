/___VBox_[a-zA-Z][a-zA-Z0-9]*_h/d
/#define/!d
/\$/d
s/#define/%define/
s/\([0-9a-fA-F][0-9a-fA-F]*\)U$/\1/
s/\([0-9a-fA-F][0-9a-fA-F]*\)U[[:space:]]/\1 /
s/[[:space:]]\/\*\*<.*$//

