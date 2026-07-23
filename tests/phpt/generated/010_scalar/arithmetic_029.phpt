--TEST--
integer arithmetic vector 029
--FILE--
<?php
$a = 122; $b = 15;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
137:107:1830:2:259:244
