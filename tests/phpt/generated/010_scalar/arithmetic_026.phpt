--TEST--
integer arithmetic vector 026
--FILE--
<?php
$a = 71; $b = 20;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
91:51:1420:11:162:284
