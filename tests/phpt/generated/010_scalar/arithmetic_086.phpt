--TEST--
integer arithmetic vector 086
--FILE--
<?php
$a = 136; $b = 12;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
148:124:1632:4:284:544
