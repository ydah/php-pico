--TEST--
associative key order vector 023
--FILE--
<?php
$items = ['left' => 33, 4 => 89, 'right' => 145];
unset($items[4]);
$items['tail'] = 146;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:33,145,146:1
