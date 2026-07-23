--TEST--
integer arithmetic vector 014
--FILE--
<?php
$a = 58; $b = 17;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
75:41:986:7:133:232
