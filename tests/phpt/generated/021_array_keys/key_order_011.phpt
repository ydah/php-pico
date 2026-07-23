--TEST--
associative key order vector 011
--FILE--
<?php
$items = ['left' => 21, 4 => 53, 'right' => 85];
unset($items[4]);
$items['tail'] = 86;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:21,85,86:1
