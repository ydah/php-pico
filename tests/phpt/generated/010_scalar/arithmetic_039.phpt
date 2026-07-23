--TEST--
integer arithmetic vector 039
--FILE--
<?php
$a = 101; $b = 6;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
107:95:606:5:208:808
