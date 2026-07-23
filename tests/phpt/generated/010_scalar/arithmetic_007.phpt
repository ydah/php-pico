--TEST--
integer arithmetic vector 007
--FILE--
<?php
$a = 130; $b = 21;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
151:109:2730:4:281:1040
