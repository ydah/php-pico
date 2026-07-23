--TEST--
array copy-on-write vector 038
--FILE--
<?php
$original = [39, 79, 119];
$copy = $original;
$copy[1] = 277; $copy[] = 278;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
39,79,119:39,277,119,278
