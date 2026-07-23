--TEST--
integer arithmetic vector 042
--FILE--
<?php
$a = 152; $b = 24;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
176:128:3648:8:328:608
