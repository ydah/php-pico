--TEST--
nested array copy-on-write vector 014
--FILE--
<?php
$tree = ['node' => ['value' => 57, 'items' => [58]]];
$copy = $tree;
$copy['node']['value'] += 2;
$copy['node']['items'][] = 61;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
57:59:58:58,61
