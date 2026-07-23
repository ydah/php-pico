--TEST--
associative key order vector 037
--FILE--
<?php
$items = ['left' => 47, 4 => 131, 'right' => 215];
unset($items[4]);
$items['tail'] = 216;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:47,215,216:1
