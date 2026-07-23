--TEST--
object clone and property COW vector 002
--FILE--
<?php
class GeneratedClone002 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone002([5, 9]);
$copy = clone $original;
$copy->items[0] = 26;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
5,9:26,9
