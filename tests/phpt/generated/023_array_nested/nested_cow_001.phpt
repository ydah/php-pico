--TEST--
nested array copy-on-write vector 001
--FILE--
<?php
$tree = ['node' => ['value' => 5, 'items' => [6]]];
$copy = $tree;
$copy['node']['value'] += 3;
$copy['node']['items'][] = 10;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
5:8:6:6,10
