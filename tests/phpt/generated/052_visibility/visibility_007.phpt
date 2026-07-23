--TEST--
private and protected member access vector 007
--FILE--
<?php
class GeneratedVisibility007 {
    private $left = 53; protected $right = 41;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility007())->total();
--EXPECT--
94
