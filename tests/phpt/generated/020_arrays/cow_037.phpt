--TEST--
array copy-on-write vector 037
--FILE--
<?php
$original = [38, 77, 116];
$copy = $original;
$copy[1] = 270; $copy[] = 271;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
38,77,116:38,270,116,271
