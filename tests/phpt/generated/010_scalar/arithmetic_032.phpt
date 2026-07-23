--TEST--
integer arithmetic vector 032
--FILE--
<?php
$a = 173; $b = 10;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
183:163:1730:3:356:173
