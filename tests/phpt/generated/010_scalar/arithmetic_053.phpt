--TEST--
integer arithmetic vector 053
--FILE--
<?php
$a = 148; $b = 21;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
169:127:3108:1:317:296
