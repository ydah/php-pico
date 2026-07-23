--TEST--
integer arithmetic vector 020
--FILE--
<?php
$a = 160; $b = 7;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
167:153:1120:6:327:160
