--TEST--
integer arithmetic vector 027
--FILE--
<?php
$a = 88; $b = 3;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
91:85:264:1:179:704
