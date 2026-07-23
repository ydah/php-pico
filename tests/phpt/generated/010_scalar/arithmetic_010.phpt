--TEST--
integer arithmetic vector 010
--FILE--
<?php
$a = 181; $b = 16;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
197:165:2896:5:378:724
