--TEST--
associative key order vector 013
--FILE--
<?php
$items = ['left' => 23, 4 => 59, 'right' => 95];
unset($items[4]);
$items['tail'] = 96;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:23,95,96:1
