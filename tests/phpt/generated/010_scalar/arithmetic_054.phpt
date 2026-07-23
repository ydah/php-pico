--TEST--
integer arithmetic vector 054
--FILE--
<?php
$a = 165; $b = 4;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
169:161:660:1:334:660
