--TEST--
array copy-on-write vector 033
--FILE--
<?php
$original = [34, 69, 104];
$copy = $original;
$copy[1] = 242; $copy[] = 243;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
34,69,104:34,242,104,243
