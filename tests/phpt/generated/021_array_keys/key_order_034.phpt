--TEST--
associative key order vector 034
--FILE--
<?php
$items = ['left' => 44, 4 => 122, 'right' => 200];
unset($items[4]);
$items['tail'] = 201;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:44,200,201:1
