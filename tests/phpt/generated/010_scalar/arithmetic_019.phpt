--TEST--
integer arithmetic vector 019
--FILE--
<?php
$a = 143; $b = 24;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
167:119:3432:23:310:1144
