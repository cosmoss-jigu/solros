AC_DEFUN([X_AC_PRBTRANS], [

AC_ARG_ENABLE([prbtrans],
  [AS_HELP_STRING([--enable-prbtrans], [build pci ring buffer transport])],
  [want_prbtrans=yes], [want_prbtrans=no])

if test x$want_prbtrans == xyes; then
  AC_DEFINE([WITH_PRBTRANS], [1], [build pci ring buffer transport])
  got_prbtrans=yes
fi

AM_CONDITIONAL([PRBTRANS], [test "x$got_prbtrans" != xno])
LIBSCIF="-lscif"

])
