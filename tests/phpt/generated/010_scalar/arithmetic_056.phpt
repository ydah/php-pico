--TEST--
integer arithmetic vector 056
--FILE--
<?php
$a = 199; $b = 16;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
215:183:3184:7:414:199
