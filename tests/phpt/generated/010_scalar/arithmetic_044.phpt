--TEST--
integer arithmetic vector 044
--FILE--
<?php
$a = 186; $b = 13;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
199:173:2418:4:385:186
