--TEST--
associative key order vector 003
--FILE--
<?php
$items = ['left' => 13, 4 => 29, 'right' => 45];
unset($items[4]);
$items['tail'] = 46;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:13,45,46:1
