--TEST--
associative key order vector 005
--FILE--
<?php
$items = ['left' => 15, 4 => 35, 'right' => 55];
unset($items[4]);
$items['tail'] = 56;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:15,55,56:1
