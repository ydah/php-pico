--TEST--
integer arithmetic vector 087
--FILE--
<?php
$a = 153; $b = 18;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
171:135:2754:9:324:1224
