m4_changequote([,])
m4_define([harcurl_version], [0.2.1])

m4_define([get_major_version], [m4_syscmd([echo $1 | cut -d. -f1])])
m4_define([get_minor_version], [m4_syscmd([echo $1 | cut -d. -f2])])
m4_define([get_micro_version], [m4_syscmd([echo $1 | cut -d. -f3])])


get_major_version(harcurl_version())
get_minor_version(harcurl_version())
get_micro_version(harcurl_version())
