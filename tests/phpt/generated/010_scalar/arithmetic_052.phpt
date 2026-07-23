--TEST--
integer arithmetic vector 052
--FILE--
<?php
$a = 131; $b = 15;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
146:116:1965:11:277:131
