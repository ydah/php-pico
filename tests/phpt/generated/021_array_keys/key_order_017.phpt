--TEST--
associative key order vector 017
--FILE--
<?php
$items = ['left' => 27, 4 => 71, 'right' => 115];
unset($items[4]);
$items['tail'] = 116;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:27,115,116:1
