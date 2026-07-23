--TEST--
integer arithmetic vector 064
--FILE--
<?php
$a = 144; $b = 18;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
162:126:2592:0:306:144
