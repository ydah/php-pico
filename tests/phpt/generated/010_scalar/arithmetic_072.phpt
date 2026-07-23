--TEST--
integer arithmetic vector 072
--FILE--
<?php
$a = 89; $b = 20;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
109:69:1780:9:198:89
