--TEST--
integer arithmetic vector 037
--FILE--
<?php
$a = 67; $b = 17;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
84:50:1139:16:151:134
