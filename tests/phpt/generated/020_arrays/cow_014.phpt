--TEST--
array copy-on-write vector 014
--FILE--
<?php
$original = [15, 31, 47];
$copy = $original;
$copy[1] = 109; $copy[] = 110;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
15,31,47:15,109,47,110
